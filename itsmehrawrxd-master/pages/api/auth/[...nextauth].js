import NextAuth from 'next-auth'
import CredentialsProvider from 'next-auth/providers/credentials'
import GoogleProvider from 'next-auth/providers/google'

export default NextAuth({
  providers: [
    CredentialsProvider({
      name: 'RawrZ Security',
      credentials: {
        username: { label: "Username", type: "text" },
        password: { label: "Password", type: "password" }
      },
      async authorize(credentials) {
        const authToken = process.env.AUTH_TOKEN
        if (credentials?.username === 'admin' && credentials?.password === authToken) {
          return {
            id: '1',
            name: 'Admin',
            email: 'admin@rawrz.security',
            role: 'admin'
          }
        }
        return null
      }
    }),
    GoogleProvider({
      clientId: process.env.GOOGLE_CLIENT_ID,
      clientSecret: process.env.GOOGLE_CLIENT_SECRET,
    })
  ],
  session: { strategy: 'jwt' },
  callbacks: {
    async jwt({ token, user, account }) {
      if (user) {
        token.role = user.role
      }
      if (account) {
        token.accessToken = account.access_token
      }
      return token
    },
    async session({ session, token }) {
      session.user.role = token.role
      session.accessToken = token.accessToken
      return session
    },
  },
  pages: {
    signIn: '/auth/signin',
    error: '/auth/error',
  }
})