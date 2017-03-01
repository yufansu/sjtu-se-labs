#include "rs.h"

const int pp[mm + 1] = { 1, 0, 0, 0, 1, 0, 0, 0, 0, 1 }; /* specify irreducible polynomial coeffts */
// const int alpha_to[nn+1] = {1, 2, 4, 8, 3, 6, 12, 11, 5, 10, 7, 14, 15, 13, 9, 0};
// const int gg[nn-kk+1] = {10, 3, 6, 13, 0};
// const int index_of[nn+1] = {-1, 0, 1, 4, 2, 8, 5, 10, 3, 14, 9, 7, 6, 13, 11, 12};

int alpha_to[nn + 1];
int gg[nn - kk + 1];
int index_of[nn + 1];

void generate_gf()
/* 生成GF(2^m)空间 */
{
	register int i, mask;
	mask = 1;
	alpha_to[mm] = 0;
	for (i = 0; i<mm; i++)
	{
		alpha_to[i] = mask;
		index_of[alpha_to[i]] = i;
		if (pp[i] != 0)
			alpha_to[mm] ^= mask;
		mask <<= 1;
	}
	index_of[alpha_to[mm]] = mm;
	mask >>= 1;
	for (i = mm + 1; i<nn; i++)
	{
		if (alpha_to[i - 1] >= mask)
			alpha_to[i] = alpha_to[mm] ^ ((alpha_to[i - 1] ^ mask) << 1);
		else
			alpha_to[i] = alpha_to[i - 1] << 1;
		index_of[alpha_to[i]] = i;
	}
	index_of[0] = -1;
	//alpha_to[nn] = 1;  
	for (i = 0; i<mm; i++)
		printf("gf%d is%d\n", i, alpha_to[i]);
}

void gen_poly()
/* 生成---生成多项式*/
{
	register int i, j;
	gg[0] = 2;    /* primitive element alpha = 2  for GF(2**mm)  */
	gg[1] = 1;    /* g(x) = (X+alpha) initially */
	for (i = 2; i <= nn - kk; i++)
	{
		gg[i] = 1;
		for (j = i - 1; j>0; j--)
			if (gg[j] != 0)
				gg[j] = gg[j - 1] ^ alpha_to[(index_of[gg[j]] + i) % nn];
			else
				gg[j] = gg[j - 1];
		gg[0] = alpha_to[(index_of[gg[0]] + i) % nn];     /* gg[0] can never be zero */
	}
	for (i = 0; i <= nn - kk; i++)
		gg[i] = index_of[gg[i]];

}

void encode_rs(int recd[nn], int data[kk], int  bb[nn-kk])
/* take the string of symbols in data[i], i=0..(k-1) and encode systematically
to produce 2*tt parity symbols in bb[0]..bb[2*tt-1]
data[] is input and bb[] is output in polynomial form.
Encoding is done by using a feedback shift register with appropriate
connections specified by the elements of gg[], which was generated above.
Codeword is c(X) = data(X)*X**(nn-kk)+ b(X) */
{
	int i,j;
	int feedback;
	for (i=0; i<nn-kk; i++)
		bb[i] = 0;
	for (i=kk-1; i>=0; i--)
	{
		feedback = index_of[data[i]^bb[nn-kk-1]];
		if (feedback != -1)
		{ 
			for (j=nn-kk-1; j>0; j--)
				if (gg[j] != -1)
					bb[j] = bb[j-1]^alpha_to[(gg[j]+feedback)%nn];
				else
					bb[j] = bb[j-1];
			bb[0] = alpha_to[(gg[0]+feedback)%nn];
		}
		else
		{
			for (j=nn-kk-1; j>0; j--)
				bb[j] = bb[j-1];
			bb[0] = 0;
		}
	}

	for (i=0; i<nn-kk; i++)
		recd[i] = bb[i];
	for (i=0; i<kk; i++) 
		recd[i+nn-kk] = data[i];
} 



void decode_rs(int recd[nn], int dataout[kk])
/* assume we have received bits grouped into mm-bit symbols in recd[i],
i=0..(nn-1), and recd[i] is index form (ie as powers of alpha).
We first compute the 2*tt syndromes by substituting alpha**i into rec(X) and
evaluating, storing the syndromes in s[i], i=1..2tt (leave s[0] zero) .
Then we use the Berlekamp iteration to find the error location polynomial
elp[i]. If the degree of the elp is >tt, we cannot correct all the errors
and hence just put out the information symbols uncorrected. If the degree of
elp is <=tt, we substitute alpha**i , i=1..n into the elp to get the roots,
hence the inverse roots, the error location numbers. If the number of errors
located does not equal the degree of the elp, we have more than tt errors
and cannot correct them. Otherwise, we then solve for the error value at
the error location and correct the error. The procedure is that found in
Lin and Costello. For the cases where the number of errors is known to be too
large to correct, the information symbols as received are output (the
advantage of systematic encoding is that hopefully some of the information
symbols will be okay and that if we are in luck, the errors are in the
parity part of the transmitted codeword). Of course, these insoluble cases
can be returned as error flags to the calling routine if desired. */
{
	int i,j,u,q ;
	int elp[nn-kk+2][nn-kk], d[nn-kk+2], l[nn-kk+2], u_lu[nn-kk+2], s[nn-kk+1] ;
	int count=0, syn_error=0, root[tt], loc[tt], z[tt+1], err[nn], reg[tt+1] ;
	/* first form the syndromes */
	for (i=1; i<=nn-kk; i++)
	{ 
		s[i] = 0 ;
		for (j=0; j<nn; j++)
			if (recd[j]!=-1)
			s[i] ^= alpha_to[(recd[j]+i*j)%nn] ; /* recd[j] in index form */
/* convert syndrome from polynomial form to index form */
		if (s[i]!=0) syn_error=1 ; /* set flag if non-zero syndrome => error */
			s[i] = index_of[s[i]] ;
	} ;
	if (syn_error) /* if errors, try and correct */
	{
/* compute the error location polynomial via the Berlekamp iterative algorithm,
following the terminology of Lin and Costello : d[u] is the 'mu'th
discrepancy, where u='mu'+1 and 'mu' (the Greek letter!) is the step number
ranging from -1 to 2*tt (see L&C), l[u] is the
degree of the elp at that step, and u_l[u] is the difference between the
step number and the degree of the elp.
*/
/* initialise table entries */
		d[0] = 0 ; /* index form */
		d[1] = s[1] ; /* index form */
		elp[0][0] = 0 ; /* index form */
		elp[1][0] = 1 ; /* polynomial form */
		for (i=1; i<nn-kk; i++)
		{ 
			elp[0][i] = -1 ; /* index form */
			elp[1][i] = 0 ; /* polynomial form */
		}
		l[0] = 0 ;
		l[1] = 0 ;
		u_lu[0] = -1 ;
		u_lu[1] = 0 ;
		u = 0 ;
		do
		{
			u++ ;
			if (d[u]==-1)
			{ 
				l[u+1] = l[u] ;
				for (i=0; i<=l[u]; i++)
					{ 
						elp[u+1][i] = elp[u][i] ;
						elp[u][i] = index_of[elp[u][i]] ;
					}
			}
			else
/* search for words with greatest u_lu[q] for which d[q]!=0 */
			{
				q = u-1 ;
				while ((d[q]==-1) && (q>0)) 
					q-- ;
/* have found first non-zero d[q] */
				if (q>0)
				{ 
					j=q ;
					do
					{ 
						j-- ;
						if ((d[j]!=-1) && (u_lu[q]<u_lu[j]))
							q = j ;
					}while (j>0) ;
				} ;
/* have now found q such that d[u]!=0 and u_lu[q] is maximum */
/* store degree of new elp polynomial */
				if (l[u]>l[q]+u-q) 
					l[u+1] = l[u] ;
				else 
					l[u+1] = l[q]+u-q ;
/* form new elp(x) */
				for (i=0; i<nn-kk; i++) 
					elp[u+1][i] = 0 ;
				for (i=0; i<=l[q]; i++)
					if (elp[q][i]!=-1)
						elp[u+1][i+u-q] = alpha_to[(d[u]+nn-d[q]+elp[q][i])%nn] ;
				for (i=0; i<=l[u]; i++)
				{
					elp[u+1][i] ^= elp[u][i] ;
					elp[u][i] = index_of[elp[u][i]] ; /*convert old elp value to index*/
				}
			}
		u_lu[u+1] = u-l[u+1] ;
/* form (u+1)th discrepancy */
		if (u<nn-kk) /* no discrepancy computed on last iteration */
		{
			if (s[u+1]!=-1)
				d[u+1] = alpha_to[s[u+1]] ;
			else
				d[u+1] = 0 ;
			for (i=1; i<=l[u+1]; i++)
			if ((s[u+1-i]!=-1) && (elp[u+1][i]!=0))
				d[u+1] = d[u+1] ^ alpha_to[(s[u+1-i]+index_of[elp[u+1][i]])%nn];

			d[u+1] = index_of[d[u+1]&0x0f] ; /* put d[u+1] into index form */
			}
		} while ((u<nn-kk) && (l[u+1]<=tt)) ;
		u++ ;
	if (l[u]<=tt) /* can correct error */
	{
/* put elp into index form */
		for (i=0; i<=l[u]; i++)
			elp[u][i] = index_of[elp[u][i]] ;
/* find roots of the error location polynomial */
		for (i=1; i<=l[u]; i++)
			reg[i] = elp[u][i] ;
		count = 0 ;
		for (i=1; i<=nn; i++)
		{ 
			q = 1 ;
			for (j=1; j<=l[u]; j++)
				if (reg[j]!=-1)
				{ 
					reg[j] = (reg[j]+j)%nn ;
					q ^= alpha_to[reg[j]] ;
				} ;
			if (!q) /* store root and error location number indices */
			{ 
				root[count] = i;
				loc[count] = nn-i ;
				count++ ;
			};
		} ;
		if (count==l[u]) /* no. roots = degree of elp hence <= tt errors */
		{
/* form polynomial z(x) */
			for (i=1; i<=l[u]; i++) /* Z[0] = 1 always - do not need */
			{
				if ((s[i]!=-1) && (elp[u][i]!=-1))
					z[i] = alpha_to[s[i]] ^ alpha_to[elp[u][i]] ;
				else if ((s[i]!=-1) && (elp[u][i]==-1))
					z[i] = alpha_to[s[i]] ;
				else if ((s[i]==-1) && (elp[u][i]!=-1))
					z[i] = alpha_to[elp[u][i]] ;
				else
					z[i] = 0 ;
			for (j=1; j<i; j++)
			if ((s[j]!=-1) && (elp[u][i-j]!=-1))
				z[i] ^= alpha_to[(elp[u][i-j] + s[j])%nn] ;
				z[i] = index_of[z[i]&0x0f] ; /* put into index form */
			} ;
/* evaluate errors at locations given by error location numbers loc[i] */
			for (i=0; i<nn; i++)
			{
				err[i] = 0 ;
				if (recd[i]!=-1) /* convert recd[] to polynomial form */
					recd[i] = alpha_to[recd[i]] ;
				else 
					recd[i] = 0 ;
			}
			for (i=0; i<l[u]; i++) /* compute numerator of error term first */
			{
				err[loc[i]] = 1; /* accounts for z[0] */
				for (j=1; j<=l[u]; j++)
					if (z[j]!=-1)
						err[loc[i]] ^= alpha_to[(z[j]+j*root[i])%nn] ;
					if (err[loc[i]]!=0)
					{ 
						err[loc[i]] = index_of[err[loc[i]]] ;
						q = 0 ; /* form denominator of error term */
						for (j=0; j<l[u]; j++)
							if (j!=i)
								q += index_of[1^alpha_to[(loc[j]+root[i])%nn]] ;
							q = q % nn ;
							err[loc[i]] = alpha_to[(err[loc[i]]-q+nn)%nn] ;
							recd[loc[i]] ^= err[loc[i]] ; /*recd[i] must be in polynomial form */
					}
			}
		}
		else /* no. roots != degree of elp => >tt errors and cannot solve */
			for (i=0; i<nn; i++) /* could return error flag if desired */
				if (recd[i]!=-1) /* convert recd[] to polynomial form */
					recd[i] = alpha_to[recd[i]] ;
				else 
					recd[i] = 0 ; /* just output received codeword as is */
		}
		else /* elp has degree has degree >tt hence cannot solve */
			for (i=0; i<nn; i++) /* could return error flag if desired */
				if (recd[i]!=-1) /* convert recd[] to polynomial form */
					recd[i] = alpha_to[recd[i]] ;
				else
					recd[i] = 0 ; /* just output received codeword as is */
	}
		else /* no non-zero syndromes => no errors: output received codeword */
			for (i=0; i<nn; i++)
				if (recd[i]!=-1) /* convert recd[] to polynomial form */
					recd[i] = alpha_to[recd[i]] ;
				else 
					recd[i] = 0 ;

	for (i=nn-kk; i<nn; i++)
	{
		dataout[i-nn+kk] = recd[i];
	}


}



void encode_rs_8(unsigned char recd[nn], unsigned char data[kk], unsigned char  bb[nn-kk])
{
	int recd1[nn], recd2[nn];
	int data1[kk], data2[kk];
	int bb1[nn-kk], bb2[nn-kk];
	int i;

	for(i = 0;i < kk;i++)
	{
		data1[i] = (int)data[i]&0x0f;
		data2[i] = (int)((data[i] >> 4)&0x0f);
	}
	encode_rs(recd1, data1, bb1);
	encode_rs(recd2, data2, bb2);

	for(i = 0;i < nn;i++)
	{
		recd[i] = (unsigned char)((recd2[i] << 4)|recd1[i]);
		
	}

	for(i = 0;i < nn-kk;i++)
	{
		bb[i] = (unsigned char)((bb2[i] << 4)|bb1[i]);
	}

}


void decode_rs_8(unsigned char recd[nn], unsigned char dataout[kk])
{
	int recd1[nn], recd2[nn];
	int dataout1[kk], dataout2[kk];
	int i;
	for(i = 0;i < nn;i++)
	{
		recd1[i] = (int)recd[i]&0x0f;
		recd2[i] = (int)((recd[i] >> 4)&0x0f);
	}

	for (i=0; i<nn; i++)
		recd1[i] = index_of[recd1[i]] ; /* put recd[i] into index form */
	decode_rs(recd1, dataout1);

	for (i=0; i<nn; i++)
		recd2[i] = index_of[recd2[i]] ; /* put recd[i] into index form */
	decode_rs(recd2, dataout2);

	for(i = 0;i < kk;i++)
	{

		dataout[i] = (unsigned char )((dataout2[i] << 4)|dataout1[i]);
	}


}